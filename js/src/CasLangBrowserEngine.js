(() => {
class CasLangError extends Error {
    constructor(code, msg) {
        super(msg);
        this.code = code;
    }
}

class CasLangBrowserEngine {
    static version = "livecoach-visible-text-nodes-2026-07-01.1";

    constructor() {
        this.vars = {};
        this.pc = 0;
        this.instructions = [];
        this.jumps = {}; // Maps pc to jump target (e.g., loop_start -> loop_end)
        this.loopStates = {}; // Stores loop iterators
        this.logs = []; // Collects str.print and str.log output
        this.session = {}; // Persistent KV state across execute() calls
    }

    async execute(jsonlString) {
        this.vars = {};
        this.pc = 0;
        this.instructions = [];
        this.jumps = {};
        this.loopStates = {};
        this.logs = [];

        // Parse JSONL
        let inBlockMode = false;
        let blockName = null;
        let blockNonce = null;
        let blockLines = [];

        const lines = jsonlString.split('\n');
        for (let i = 0; i < lines.length; i++) {
            const rawLine = lines[i];
            const line = rawLine.trim();
            if (!inBlockMode && !line) continue;
            if (!inBlockMode && line.startsWith('//')) continue;
            
            if (inBlockMode) {
                // Check if this line is the terminator
                let isTerminator = false;
                if (line.startsWith('{') && line.endsWith('}')) {
                    try {
                        const j = JSON.parse(line);
                        if (j.op === 'flow.end_set' && j.name === blockName && j.nonce === blockNonce) {
                            isTerminator = true;
                        }
                    } catch(e) {}
                }
                
                if (isTerminator) {
                    this.instructions.push({
                        op: 'flow.set',
                        name: blockName,
                        value: blockLines.join('\n')
                    });
                    inBlockMode = false;
                    blockName = null;
                    blockNonce = null;
                    blockLines = [];
                } else {
                    // Preserve original whitespace for block lines
                    blockLines.push(rawLine);
                }
                continue;
            }

            try {
                const inst = JSON.parse(line);
                if (inst.op === 'flow.set' && inst.mode === 'block') {
                    if (!inst.nonce) throw new CasLangError("E1003", "E_COMMAND_SYNTAX: Block mode requires nonce");
                    inBlockMode = true;
                    blockName = inst.name;
                    blockNonce = inst.nonce;
                    blockLines = [];
                } else {
                    this.instructions.push(inst);
                }
            } catch (e) {
                if (e instanceof CasLangError) throw e;
                throw new CasLangError("E1004", "E_JSON_INVALID: " + e.message + " on line: " + line);
            }
        }

        if (inBlockMode) {
            throw new CasLangError("E2301", "E_BLOCK_UNBALANCED: Block set never terminated.");
        }

        this._buildJumpTable();

        // Execution Loop
        while (this.pc < this.instructions.length) {
            const inst = this.instructions[this.pc];
            
            if (!inst.op) {
                throw new CasLangError("E1003", "E_COMMAND_SYNTAX: Missing 'op' field.");
            }

            try {
                const ns = inst.op.split('.')[0];
                if (inst.op === 'caslang') { this.pc++; }
                else if (ns === 'flow') await this._opFlow(inst);
                else if (ns === 'list') await this._opList(inst);
                else if (ns === 'dict') await this._opDict(inst);
                else if (ns === 'str') await this._opStr(inst);
                else if (ns === 'time') await this._opTime(inst);
                else if (ns === 'json') await this._opJson(inst);
                else if (ns === 'browser') await this._opBrowser(inst);
                else if (ns === 'session') await this._opSession(inst);
                else {
                    throw new CasLangError("E2001", "E_OP_UNKNOWN: " + inst.op);
                }
            } catch (e) {
                if (e && e.isReturn) return { result: e.value, logs: this.logs }; // Return both result and collected logs
                if (e instanceof CasLangError) throw e;
                throw new CasLangError("E3001", `E_RUNTIME_EXCEPTION at PC ${this.pc} [${inst.op}]: ` + (e.message || e));
            }
            
            // Note: flow.return will throw a special 'ReturnException' to break out cleanly
        }

        return { result: null, logs: this.logs };
    }

    _buildJumpTable() {
        const stack = [];
        for (let i = 0; i < this.instructions.length; i++) {
            const op = this.instructions[i].op;
            if (op === 'flow.loop_start' || op === 'flow.if') {
                stack.push({ type: op, index: i });
            } else if (op === 'flow.else') {
                // Must attach to an if
                if (stack.length === 0 || stack[stack.length - 1].type !== 'flow.if') {
                    throw new CasLangError("E2301", "E_BLOCK_UNBALANCED: flow.else without flow.if");
                }
                const ifBlock = stack[stack.length - 1];
                this.jumps[ifBlock.index] = i; // if -> else
                // Update stack top to be the else block
                ifBlock.type = 'flow.else';
                ifBlock.index = i;
            } else if (op === 'flow.loop_end' || op === 'flow.endif') {
                if (stack.length === 0) throw new CasLangError("E2301", "E_BLOCK_UNBALANCED: " + op);
                const start = stack.pop();
                
                if ((op === 'flow.loop_end' && start.type !== 'flow.loop_start') ||
                    (op === 'flow.endif' && (start.type !== 'flow.if' && start.type !== 'flow.else'))) {
                    throw new CasLangError("E2301", "E_BLOCK_UNBALANCED: Mismatched block end " + op);
                }
                
                this.jumps[start.index] = i; // start -> end
                this.jumps[i] = start.index; // end -> start (for loops)
            }
        }
        if (stack.length > 0) throw new CasLangError("E2301", "E_BLOCK_UNBALANCED: Unclosed blocks remain.");
    }

    _resolve(val) {
        if (typeof val !== 'string') return val;
        
        // Exact match mapping (returns raw object, e.g. DOM Element or Array)
        const exactMatch = /^\$\{([^}]+)\}$/.exec(val);
        if (exactMatch) {
            return this._getVar(exactMatch[1]);
        }

        // Interpolation
        return val.replace(/\$\{([^}]+)\}/g, (match, path) => {
            const v = this._getVar(path);
            return (typeof v === 'object') ? JSON.stringify(v) : String(v);
        });
    }

    _getVar(path) {
        // Handle basic access: varName, varName[0], varName['key']
        // This is a simplified resolver. A real one parses bracket access properly.
        const parts = path.split(/\[(.*?)\]/).filter(Boolean);
        const rootName = parts[0];
        
        if (!(rootName in this.vars)) {
            throw new CasLangError("E2201", "E_VAR_UNDEFINED: " + rootName);
        }
        
        let curr = this.vars[rootName];
        for (let i = 1; i < parts.length; i++) {
            let key = parts[i];
            // Remove quotes if present
            if ((key.startsWith("'") && key.endsWith("'")) || (key.startsWith('"') && key.endsWith('"'))) {
                key = key.substring(1, key.length - 1);
            }
            // Resolve nested variables in keys, e.g., ${idx} inside bracket
            if (key.startsWith('${') && key.endsWith('}')) {
                key = this._resolve(key);
            }
            
            if (curr === null || curr === undefined) throw new CasLangError("E2411", "Cannot access property of null/undefined");
            curr = curr[key];
        }
        return curr;
    }

    _setVar(name, value) {
        if (!name) throw new CasLangError("E2101", "E_ARG_MISSING: 'name' is required for set");
        this.vars[name] = value;
    }

    _refName(ref) {
        if (typeof ref !== 'string') return null;
        const trimmed = ref.trim();
        const exactMatch = /^\$\{([^}]+)\}$/.exec(trimmed);
        if (exactMatch) return exactMatch[1].trim();
        if (trimmed.startsWith("${") && trimmed.endsWith("}")) {
            return trimmed.substring(2, trimmed.length - 1).trim();
        }
        return null;
    }

    _getExactRef(ref) {
        const name = this._refName(ref);
        if (!name) return { name: null, value: this._resolve(ref) };
        return { name, value: this.vars[name] };
    }

    _toList(value) {
        if (Array.isArray(value)) return value;
        if (value === null || value === undefined || typeof value === 'string') return null;
        if (typeof NodeList !== 'undefined' && value instanceof NodeList) return Array.from(value);
        if (typeof HTMLCollection !== 'undefined' && value instanceof HTMLCollection) return Array.from(value);
        if (typeof value.length === 'number') return Array.from(value);
        if (typeof value[Symbol.iterator] === 'function') return Array.from(value);
        return null;
    }

    _isDomNode(value) {
        if (!value || typeof value !== 'object') return false;
        if (typeof Node !== 'undefined' && value instanceof Node) return true;
        return typeof value.nodeType === 'number' && typeof value.nodeName === 'string';
    }

    _isDict(value) {
        return value !== null && typeof value === 'object' && !Array.isArray(value) && !this._isDomNode(value);
    }

    _debugType(value) {
        if (value === null) return "null";
        if (value === undefined) return "undefined";
        if (Array.isArray(value)) return "array";
        if (this._isDomNode(value)) return "dom-node";
        return typeof value;
    }

    _queryRoot(inst) {
        if (!inst.element) return document;
        const root = this._resolve(inst.element);
        if (!this._isDomNode(root) || typeof root.querySelector !== 'function') {
            throw new CasLangError("E2103", "E_ARG_TYPE: element is not a queryable DOM node");
        }
        return root;
    }

    // --- NAMESPACES ---

    async _opFlow(inst) {
        if (inst.op === 'flow.set') {
            let val = inst.value;
            if (typeof val === 'string' && val.startsWith('=')) {
                // Expression mode (simple math only, no eval for security)
                val = this._resolveExpression(val.substring(1).trim());
            } else if (typeof val === 'string') {
                val = this._resolve(val);
            }
            this._setVar(inst.name, val);
            this.pc++;
        }
        else if (inst.op === 'flow.if') {
            const condString = this._resolve(inst.cond);
            const isTrue = this._evaluateBinaryCondition(condString);
            
            if (!isTrue) {
                // Jump to else or endif
                this.pc = this.jumps[this.pc] + 1; // Jump past the end block
            } else {
                this.pc++;
            }
        }
        else if (inst.op === 'flow.else') {
            // If we naturally hit an else, it means the 'if' was true and we executed it.
            // We must jump past the endif.
            this.pc = this.jumps[this.pc] + 1;
        }
        else if (inst.op === 'flow.endif') {
            this.pc++;
        }
        else if (inst.op === 'flow.loop_start') {
            const list = this._toList(this._resolve(inst.in));
            if (!list) {
                throw new CasLangError("E2103", "E_ARG_TYPE: flow.loop_start 'in' must be a list/NodeList");
            }
            
            if (!this.loopStates[this.pc]) {
                this.loopStates[this.pc] = { idx: 0 };
            }
            
            const state = this.loopStates[this.pc];
            if (state.idx < list.length) {
                this._setVar(inst.var, list[state.idx]);
                state.idx++;
                this.pc++;
            } else {
                // Loop done
                delete this.loopStates[this.pc];
                this.pc = this.jumps[this.pc] + 1;
            }
        }
        else if (inst.op === 'flow.loop_end') {
            // Jump back to start
            this.pc = this.jumps[this.pc];
        }
        else if (inst.op === 'flow.return') {
            const val = this._resolve(inst.value);
            // Halt execution and throw standard object to break loop
            throw { isReturn: true, value: val };
        }
        else {
            throw new CasLangError("E2001", "E_OP_UNKNOWN: " + inst.op);
        }
    }

    _evaluateBinaryCondition(str) {
        if (str === 'true' || str === true) return true;
        if (str === 'false' || str === false || str === 'null' || str === null || str === '') return false;
        
        const ops = ['==', '!=', '<=', '>=', '<', '>'];
        let op = null;
        for (const o of ops) {
            if (str.includes(` ${o} `)) {
                op = o;
                break;
            }
        }
        
        if (!op) return !!str; // Truthy check

        const parts = str.split(` ${op} `);
        const left = isNaN(parts[0]) ? parts[0] : Number(parts[0]);
        const right = isNaN(parts[1]) ? parts[1] : Number(parts[1]);

        switch (op) {
            case '==': return left == right;
            case '!=': return left != right;
            case '<': return left < right;
            case '>': return left > right;
            case '<=': return left <= right;
            case '>=': return left >= right;
        }
        return false;
    }

    _resolveExpression(str) {
        // Minimal math evaluator avoiding eval()
        // e.g. "5 + 3" -> 8
        const resolved = this._resolve(str);
        // Extremely simple parser for basic arithmetic (x + y)
        const match = resolved.match(/^([\d.]+)\s*([\+\-\*\/])\s*([\d.]+)$/);
        if (match) {
            const left = Number(match[1]);
            const op = match[2];
            const right = Number(match[3]);
            if (op === '+') return left + right;
            if (op === '-') return left - right;
            if (op === '*') return left * right;
            if (op === '/') return left / right;
        }
        return resolved; // Fallback
    }

    // List Ops
    async _opList(inst) {
        if (inst.op === 'list.new') {
            this._setVar(inst.as || inst.name, []);
        } else if (inst.op === 'list.append') {
            const ref = this._getExactRef(inst.list);
            let lst = ref.value;
            if (!Array.isArray(lst)) {
                const name = ref.name;
                if (name && (
                    lst === null ||
                    lst === undefined ||
                    (typeof lst === 'string' && lst.trim() === 'null')
                )) {
                    lst = [];
                    this._setVar(name, lst);
                } else if (name) {
                    const normalized = this._toList(lst);
                    if (!normalized) {
                        lst = [];
                        this._setVar(name, lst);
                    } else {
                        lst = normalized;
                        this._setVar(name, lst);
                    }
                } else {
                    throw new CasLangError("E2103", "E_ARG_TYPE: Target is not a list");
                }
            }
            lst.push(this._resolve(inst.value !== undefined ? inst.value : inst.item));
        } else if (inst.op === 'list.slice') {
            const lst = this._toList(this._resolve(inst.list));
            if (!lst) throw new CasLangError("E2103", "E_ARG_TYPE: Target is not a list/NodeList");
            const start = this._resolve(inst.start) || 0;
            const end = this._resolve(inst.end);
            
            this._setVar(inst.as, lst.slice(start, end !== undefined ? end : lst.length));
        } else {
            throw new CasLangError("E2001", "Unsupported list op: " + inst.op);
        }
        this.pc++;
    }

    // Dict Ops
    async _opDict(inst) {
        if (inst.op === 'dict.new') {
            this._setVar(inst.as || inst.name, {});
        } else if (inst.op === 'dict.set') {
            const ref = this._getExactRef(inst.dict);
            let dict = ref.value;
            const name = ref.name;
            if (name && !this._isDict(dict)) {
                dict = {};
                this._setVar(name, dict);
            }
            if (!this._isDict(dict)) {
                throw new CasLangError(
                    "E2103",
                    `E_ARG_TYPE: Target is not a dict ref=${String(inst.dict)} name=${name || ""} type=${this._debugType(dict)}`
                );
            }
            let key = String(this._resolve(inst.key));
            if (key.startsWith("'") && key.endsWith("'")) key = key.substring(1, key.length - 1);
            dict[key] = this._resolve(inst.value);
        } else if (inst.op === 'dict.has') {
            const ref = this._getExactRef(inst.dict);
            const dict = ref.value;
            if (!this._isDict(dict)) throw new CasLangError("E2103", "E_ARG_TYPE: Target is not a dict");
            let key = String(this._resolve(inst.key));
            if (key.startsWith("'") && key.endsWith("'")) key = key.substring(1, key.length - 1);
            this._setVar(inst.as, key in dict);
        } else {
            throw new CasLangError("E2001", "Unsupported dict op: " + inst.op);
        }
        this.pc++;
    }

    // Str Ops
    async _opStr(inst) {
        if (inst.op === 'str.contains') {
            const s = String(this._resolve(inst.s || inst.text));
            const sub = String(this._resolve(inst.sub || inst.substr));
            this._setVar(inst.as, s.includes(sub));
        } else if (inst.op === 'str.replace') {
            const s = String(this._resolve(inst.s || inst.text));
            const oldStr = String(this._resolve(inst.old));
            const newStr = String(this._resolve(inst.new));
            this._setVar(inst.as, s.split(oldStr).join(newStr));
        } else if (inst.op === 'str.print' || inst.op === 'str.log') {
            this.logs.push(String(this._resolve(inst.msg)));
        } else {
            throw new CasLangError("E2001", "Unsupported str op: " + inst.op);
        }
        this.pc++;
    }

    // Time Ops
    async _opTime(inst) {
        if (inst.op === 'time.sleep') {
            await new Promise(r => setTimeout(r, Number(this._resolve(inst.ms))));
        } else {
            throw new CasLangError("E2001", "Unsupported time op: " + inst.op);
        }
        this.pc++;
    }

    // Session KV Ops
    async _opSession(inst) {
        if (inst.op === 'session.set') {
            this.session[inst.key] = this._resolve(inst.val);
        } else if (inst.op === 'session.get') {
            this._setVar(inst.as, this.session[inst.key]);
        } else if (inst.op === 'session.clear') {
            if (inst.key) {
                delete this.session[inst.key];
            } else {
                this.session = {};
            }
        } else {
            throw new CasLangError("E2001", "Unsupported session op: " + inst.op);
        }
        this.pc++;
    }

    // Json Ops
    async _opJson(inst) {
        if (inst.op === 'json.parse') {
            const str = this._resolve(inst.s);
            this._setVar(inst.as, JSON.parse(str));
        } else {
            throw new CasLangError("E2001", "Unsupported json op: " + inst.op);
        }
        this.pc++;
    }

    // Browser Ops
    async _opBrowser(inst) {
        const collectVisibleTextNodes = () => {
            const maxNodes = Number(this._resolve(inst.max || 300));
            const chunkSize = Number(this._resolve(inst.chunk_size || inst.max_chars_per_node || 700));
            const includeOffscreen = Boolean(inst.include_offscreen);
            const store = inst.store !== false;
            const scanId = String(inst.scan_id ? this._resolve(inst.scan_id) : ("txtscan_" + Date.now() + "_" + Math.random().toString(36).slice(2, 8)));
            const viewportW = window.innerWidth || document.documentElement.clientWidth || 0;
            const viewportH = window.innerHeight || document.documentElement.clientHeight || 0;
            const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, {
                acceptNode: (node) => {
                    const text = (node.nodeValue || '').replace(/\s+/g, ' ').trim();
                    if (!text) return NodeFilter.FILTER_REJECT;
                    if (!node.parentElement) return NodeFilter.FILTER_REJECT;
                    const parent = node.parentElement;
                    const tag = parent.tagName ? parent.tagName.toLowerCase() : '';
                    if (['script', 'style', 'noscript', 'template'].includes(tag)) return NodeFilter.FILTER_REJECT;
                    if (parent.closest('[hidden],[aria-hidden="true"]')) return NodeFilter.FILTER_REJECT;
                    const style = window.getComputedStyle(parent);
                    if (style.display === 'none' || style.visibility === 'hidden' || Number(style.opacity) === 0) {
                        return NodeFilter.FILTER_REJECT;
                    }
                    const rect = parent.getBoundingClientRect();
                    if (!rect || rect.width <= 0 || rect.height <= 0) return NodeFilter.FILTER_REJECT;
                    const inViewport = rect.bottom >= 0 && rect.right >= 0 && rect.top <= viewportH && rect.left <= viewportW;
                    if (!includeOffscreen && !inViewport) return NodeFilter.FILTER_REJECT;
                    return NodeFilter.FILTER_ACCEPT;
                }
            });
            const nodes = [];
            const fullNodes = [];
            const seen = new Set();
            let node;
            while ((node = walker.nextNode()) && nodes.length < maxNodes) {
                const parent = node.parentElement;
                const text = (node.nodeValue || '').replace(/\s+/g, ' ').trim();
                if (!text || seen.has(text)) continue;
                seen.add(text);
                const rect = parent.getBoundingClientRect();
                const style = window.getComputedStyle(parent);
                const inViewport = rect.bottom >= 0 && rect.right >= 0 && rect.top <= viewportH && rect.left <= viewportW;
                const nodeId = "n" + fullNodes.length;
                const meta = {
                    source_hint: 'visible_text_node',
                    evidence: 'Visible browser text node collected with TreeWalker.',
                    confidence: inViewport ? 0.76 : 0.42,
                    scan_id: scanId,
                    node_id: nodeId,
                    chunk_index: 0,
                    offset_start: 0,
                    offset_end: Math.min(text.length, chunkSize),
                    original_length: text.length,
                    has_more: text.length > chunkSize,
                    tag: parent.tagName ? parent.tagName.toLowerCase() : '',
                    role: parent.getAttribute('role') || '',
                    aria_label: parent.getAttribute('aria-label') || '',
                    placeholder: parent.getAttribute('placeholder') || '',
                    rect: {
                        x: Math.round(rect.x),
                        y: Math.round(rect.y),
                        w: Math.round(rect.width),
                        h: Math.round(rect.height)
                    },
                    font_size: style.fontSize || '',
                    font_weight: style.fontWeight || '',
                    in_viewport: inViewport
                };
                fullNodes.push({ text, meta });
                nodes.push({
                    text: text.slice(0, chunkSize),
                    ...meta
                });
            }
            nodes.sort((a, b) => {
                if (a.in_viewport !== b.in_viewport) return a.in_viewport ? -1 : 1;
                if (a.rect.y !== b.rect.y) return a.rect.y - b.rect.y;
                return a.rect.x - b.rect.x;
            });
            if (store) {
                if (!this.session.livecoach_text_scans) this.session.livecoach_text_scans = {};
                this.session.livecoach_text_scans[scanId] = {
                    created_at: Date.now(),
                    url: location.href,
                    title: document.title,
                    nodes: fullNodes
                };
                const scanIds = Object.keys(this.session.livecoach_text_scans)
                    .sort((a, b) => this.session.livecoach_text_scans[a].created_at - this.session.livecoach_text_scans[b].created_at);
                while (scanIds.length > 3) {
                    const oldId = scanIds.shift();
                    delete this.session.livecoach_text_scans[oldId];
                }
                this.session.livecoach_latest_text_scan_id = scanId;
            }
            return nodes;
        };

        const getTextNodeChunks = () => {
            const scanId = String(inst.scan_id ? this._resolve(inst.scan_id) : (this.session.livecoach_latest_text_scan_id || ''));
            const scans = this.session.livecoach_text_scans || {};
            const scan = scans[scanId];
            if (!scan) {
                throw new CasLangError("E3001", "Text node scan not found: " + scanId);
            }
            const requests = this._resolve(inst.requests || []);
            const maxChars = Number(this._resolve(inst.max_total_chars || 20000));
            const chunks = [];
            let used = 0;
            for (const req of Array.isArray(requests) ? requests : []) {
                const nodeId = String(req.node_id || '');
                const nodeIndex = Number(nodeId.replace(/^n/, ''));
                const entry = scan.nodes[nodeIndex];
                if (!entry) continue;
                const fullText = entry.text || '';
                const start = Math.max(0, Number(req.offset_start || 0));
                const requestedEnd = req.offset_end == null ? fullText.length : Number(req.offset_end);
                const end = Math.min(fullText.length, requestedEnd, start + Math.max(0, maxChars - used));
                if (end <= start) continue;
                const text = fullText.slice(start, end);
                used += text.length;
                chunks.push({
                    text,
                    source_hint: 'visible_text_node_chunk',
                    evidence: 'Expanded visible browser text node chunk from TreeWalker session cache.',
                    confidence: entry.meta?.confidence || 0.6,
                    scan_id: scanId,
                    node_id: nodeId,
                    offset_start: start,
                    offset_end: end,
                    original_length: fullText.length,
                    has_more: end < fullText.length,
                    tag: entry.meta?.tag || '',
                    role: entry.meta?.role || '',
                    rect: entry.meta?.rect || null,
                    in_viewport: Boolean(entry.meta?.in_viewport)
                });
                if (used >= maxChars) break;
            }
            return chunks;
        };

        const getEl = (inst) => {
            if (inst.element) {
                const el = this._resolve(inst.element);
                if (!this._isDomNode(el)) {
                    throw new CasLangError("E2103", "E_ARG_TYPE: element is not a DOM node");
                }
                return el;
            }
            if (inst.selector) {
                const el = document.querySelector(this._resolve(inst.selector));
                if (!el) throw new CasLangError("E3001", "Element not found: " + inst.selector);
                return el;
            }
            throw new CasLangError("E2101", "E_ARG_MISSING: element or selector required");
        };

        if (inst.op === 'browser.query') {
            const parent = this._queryRoot(inst);
            const el = parent.querySelector(this._resolve(inst.selector));
            this._setVar(inst.as, el);
        } 
        else if (inst.op === 'browser.query_all') {
            const parent = this._queryRoot(inst);
            const els = Array.from(parent.querySelectorAll(this._resolve(inst.selector)));
            this._setVar(inst.as, els);
        }
        else if (inst.op === 'browser.visible_text_nodes') {
            this._setVar(inst.as, collectVisibleTextNodes());
        }
        else if (inst.op === 'browser.get_text_node_chunks') {
            this._setVar(inst.as, getTextNodeChunks());
        }
        else if (inst.op === 'browser.parent') {
            this._setVar(inst.as, getEl(inst).parentElement);
        }
        else if (inst.op === 'browser.children') {
            this._setVar(inst.as, Array.from(getEl(inst).children));
        }
        else if (inst.op === 'browser.siblings') {
            const el = getEl(inst);
            if (!el.parentElement) this._setVar(inst.as, []);
            else this._setVar(inst.as, Array.from(el.parentElement.children).filter(c => c !== el));
        }
        else if (inst.op === 'browser.get_text') {
            const el = getEl(inst);
            this._setVar(inst.as, el.innerText || el.textContent);
        }
        else if (inst.op === 'browser.get_value') {
            this._setVar(inst.as, getEl(inst).value);
        }
        else if (inst.op === 'browser.set_value') {
            getEl(inst).value = this._resolve(inst.val);
        }
        else if (inst.op === 'browser.get_html') {
            this._setVar(inst.as, getEl(inst).innerHTML);
        }
        else if (inst.op === 'browser.set_html') {
            getEl(inst).innerHTML = this._resolve(inst.html);
        }
        else if (inst.op === 'browser.get_attr') {
            this._setVar(inst.as, getEl(inst).getAttribute(this._resolve(inst.attr)));
        }
        else if (inst.op === 'browser.set_attr') {
            getEl(inst).setAttribute(this._resolve(inst.attr), this._resolve(inst.val));
        }
        else if (inst.op === 'browser.remove_attr') {
            getEl(inst).removeAttribute(this._resolve(inst.attr));
        }
        else if (inst.op === 'browser.add_class') {
            getEl(inst).classList.add(this._resolve(inst.class));
        }
        else if (inst.op === 'browser.remove_class') {
            getEl(inst).classList.remove(this._resolve(inst.class));
        }
        else if (inst.op === 'browser.has_class') {
            this._setVar(inst.as, getEl(inst).classList.contains(this._resolve(inst.class)));
        }
        else if (inst.op === 'browser.get_style') {
            this._setVar(inst.as, getEl(inst).style[this._resolve(inst.prop)] || window.getComputedStyle(getEl(inst))[this._resolve(inst.prop)]);
        }
        else if (inst.op === 'browser.set_style') {
            getEl(inst).style[this._resolve(inst.prop)] = this._resolve(inst.val);
        }
        else if (inst.op === 'browser.click') {
            getEl(inst).click();
        }
        else if (inst.op === 'browser.type') {
            const el = getEl(inst);
            el.focus();
            document.execCommand('insertText', false, this._resolve(inst.text));
        }
        else if (inst.op === 'browser.hover') {
            const el = getEl(inst);
            el.dispatchEvent(new MouseEvent('mouseover', { bubbles: true }));
        }
        else if (inst.op === 'browser.focus') {
            getEl(inst).focus();
        }
        else if (inst.op === 'browser.blur') {
            getEl(inst).blur();
        }
        else if (inst.op === 'browser.scroll_to') {
            getEl(inst).scrollIntoView({ behavior: 'smooth', block: 'center' });
        }
        else {
            throw new CasLangError("E2001", "Unsupported browser op: " + inst.op);
        }
        this.pc++;
    }
}

globalThis.CasLangBrowserEngine = CasLangBrowserEngine;
globalThis.CasLangError = CasLangError;
})();
