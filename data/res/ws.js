let socket = new WebSocket('ws://'+ getCurrentUrl());

socket.onopen = function(e) {
    log('Connection established!');
};

socket.onmessage = function(event) {
    log('Message from server ', event.data);
    // update 
};

socket.onclose = function(event) {
    if (event.wasClean) {
        log('Connection closed cleanly');
    } else {
        log('Connection died');
    }
    log('Code: ' + event.code + ' reason: ' + event.reason);
};