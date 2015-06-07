var files = require('fs').readdirSync(__dirname).filter(function (name) {
    return /\.js$/.test(name) && name !== 'index.js';
});

var fork = require('child_process').fork;

sched();

function sched() {
    if (!files.length) return;
    var name = files.shift();
    console.log('\x1b[32mfork\x1b[0m ' + name);
    fork(__dirname + '/' + name).on('exit', sched);

}
