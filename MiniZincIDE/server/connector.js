const MiniZincIDE = (() => {
    const callbacks = {};
    const responses = [];
    const freeSlots = [];
    let userData;
    
    window.addEventListener('message', (e) => {
        switch (e.data.event) {
            case 'response': {
                resolveResponse(e.data.id, e.data.payload);
                break;
            }
            case 'error': {
                rejectResponse(e.data.id, e.data.message);
                break;
            }
            default:
                if (e.data.event in callbacks) {
                    callbacks[e.data.event].forEach(callback => {
                        callback(e.data.payload);
                    });
                }
                break;
        }
    });

    function resolveResponse(index, payload) {
        const { resolve } = responses[index];
        resolve(payload);
        responses[index] = null;
        freeSlots.push(index);
    }

    function rejectResponse(index, message) {
        const { reject } = responses[index];
        reject(message);
        responses[index] = null;
        freeSlots.push(index);
    }

    function createPromise(message) {
        return new Promise((resolve, reject) => {
            const id = freeSlots.length > 0 ? freeSlots.pop() : responses.length;
            responses[id] = {resolve, reject};
            window.parent.postMessage({
                ...message,
                id
            }, '*');
        });
    }
    
    function on(event, callback) {
        if (!(event in callbacks)) {
            callbacks[event] = new Set();
        }
        callbacks[event].add(callback);
    }
    function off(event, callback) {
        if (event in callbacks) {
            callbacks[event].delete(callback);
        }
    }
    function getUserData() {
        return new Promise((resolve, reject) => {
            if (userData === undefined) {
                on('init', resolve);
            } else {
                resolve(userData);
            }
        });
    }
    function goToSolution(idx) {
        window.parent.postMessage({
            event: 'rebroadcast',
            message: {
                event: 'goToSolution',
                payload: idx
            }
        }, '*');
    }
    function solve(model, data, options) {
        window.parent.postMessage({
            event: 'solve',
            model,
            data,
            options
        }, '*');
    }
    function getNumSolutions() {
        return createPromise({
            event: 'getNumSolutions'
        });
    }
    function getSolution(index) {
        return createPromise({
            event: 'getSolution',
            index
        });
    }
    function getAllSolutions() {
        return createPromise({
            event: 'getAllSolutions'
        });
    }
    function getStatus() {
        return createPromise({
            event: 'getStatus'
        });
    }
    function getFinishTime() {
        return createPromise({
            event: 'getFinishTime'
        });
    }

    return {
        getUserData,
        on,
        off,
        goToSolution,
        solve,
        getNumSolutions,
        getSolution,
        getAllSolutions,
        getStatus,
        getFinishTime
    };
})();
