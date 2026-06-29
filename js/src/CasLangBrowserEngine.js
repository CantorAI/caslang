class CasLangError extends Error {
    constructor(code, msg) {
        super(msg);
        this.code = code;
    }
}

class CasLangBrowserEngine {
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
            const list = this._resolve(inst.in);
            if (!Array.isArray(list) && !(list instanceof NodeList)) {
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
            this._setVar(inst.as, []);
        } else if (inst.op === 'list.append') {
            const lst = this._resolve(inst.list);
            if (!Array.isArray(lst)) throw new CasLangError("E2103", "E_ARG_TYPE: Target is not a list");
            lst.push(this._resolve(inst.item));
        } else if (inst.op === 'list.slice') {
            const lst = this._resolve(inst.list);
            if (!Array.isArray(lst) && !(lst instanceof NodeList)) throw new CasLangError("E2103", "E_ARG_TYPE: Target is not a list/NodeList");
            const start = this._resolve(inst.start) || 0;
            const end = this._resolve(inst.end);
            
            const arr = Array.from(lst);
            this._setVar(inst.as, arr.slice(start, end !== undefined ? end : arr.length));
        } else {
            throw new CasLangError("E2001", "Unsupported list op: " + inst.op);
        }
        this.pc++;
    }

    // Dict Ops
    async _opDict(inst) {
        if (inst.op === 'dict.new') {
            this._setVar(inst.as, {});
        } else if (inst.op === 'dict.set') {
            const dict = this._resolve(inst.dict);
            let key = this._resolve(inst.key);
            if (key.startsWith("'") && key.endsWith("'")) key = key.substring(1, key.length - 1);
            dict[key] = this._resolve(inst.value);
        } else if (inst.op === 'dict.has') {
            const dict = this._resolve(inst.dict);
            let key = this._resolve(inst.key);
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
        const getEl = (inst) => {
            if (inst.element) {
                const el = this._resolve(inst.element);
                if (!(el instanceof Element || el instanceof Node)) {
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
            const parent = inst.element ? this._resolve(inst.element) : document;
            const el = parent.querySelector(this._resolve(inst.selector));
            this._setVar(inst.as, el);
        } 
        else if (inst.op === 'browser.query_all') {
            const parent = inst.element ? this._resolve(inst.element) : document;
            const els = Array.from(parent.querySelectorAll(this._resolve(inst.selector)));
            this._setVar(inst.as, els);
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
