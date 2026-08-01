/*
Copyright (C) 2026 CantorAI Inc.
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

document.addEventListener('DOMContentLoaded', () => {
    const runBtn = document.getElementById('run-btn');
    const scriptInput = document.getElementById('script-input');
    const consoleOutput = document.getElementById('console-output');

    // Add event listeners to mock DOM buttons so we can see when they are clicked
    document.querySelectorAll('.delete-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            const row = e.target.closest('tr');
            row.style.backgroundColor = '#ffcccc';
            setTimeout(() => {
                row.remove();
                logToConsole(`Mock DOM Event: Deleted row containing '${row.cells[1].innerText}'`);
            }, 300);
        });
    });

    function logToConsole(msg, isError = false) {
        const line = document.createElement('div');
        line.textContent = `> ${msg}`;
        if (isError) line.style.color = '#ff5555';
        consoleOutput.appendChild(line);
        consoleOutput.scrollTop = consoleOutput.scrollHeight;
    }

    runBtn.addEventListener('click', async () => {
        consoleOutput.innerHTML = ''; // Clear console
        const scriptText = scriptInput.value.trim();
        
        if (!scriptText) {
            logToConsole('Error: Script is empty.', true);
            return;
        }

        logToConsole('Starting execution...');
        
        try {
            // Instantiate engine and run
            const engine = new CasLangBrowserEngine();
            const output = await engine.execute(scriptText);
            
            if (output.logs && output.logs.length > 0) {
                logToConsole(`--- Collected Logs ---`);
                output.logs.forEach(l => logToConsole(`[LOG] ${l}`));
                logToConsole(`----------------------`);
            }
            
            logToConsole(`Execution completed successfully.`);
            logToConsole(`Result: ${JSON.stringify(output.result, null, 2)}`);
        } catch (error) {
            logToConsole(`EXECUTION HALTED: ${error.message || error}`, true);
            if (error.code) {
                logToConsole(`Error Code: ${error.code}`, true);
            }
        }
    });
});
