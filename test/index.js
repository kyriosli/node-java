var files = require('fs').readdirSync(__dirname).filter(function (name) {
    return /\.js$/.test(name) && name !== 'index.js';
});

var fork = require('child_process').fork;

var prev = 0;

sched();

function sched() {
    if (!files.length) return;
    var name = files.shift();
    var now = Date.now();
    if(prev) {
    	console.log('\x1b[36mdone\x1b[0m: ' + (now - prev) + 'ms');
    }
    prev = now;
    console.log('\x1b[32mfork\x1b[0m ' + name);
    fork(__dirname + '/' + name).on('exit', sched);

}
