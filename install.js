var pkg = require('./package.json');
var host = pkg.author + '.github.io';
var options = {
    method: 'GET',
    host: host,
    path: '/' + pkg.name + '/dists/' + process.platform + '-' + process.arch + '-' + process.versions.modules + '.gz',
    headers: {
        Host: host,
        'Accept-Encoding': 'gzip, deflate',
        Connection: 'close'
    }
};

require('http').request(options, function (tres) {
    if (tres.statusCode !== 200) {
        return onerror({message: 'file not found'});
    }
    var zlib = require('zlib');
    if (tres.headers['content-encoding'] === 'gzip') {
        tres = tres.pipe(zlib.createGunzip())
    } else if (tres.headers['content-encoding'] === 'deflate') {
        tres = tres.pipe(zlib.createInflateRaw())
    }
    tres = tres.pipe(zlib.createGunzip());
    var fs = require('fs');
    if (!fs.existsSync('build/Release')) {
        fs.existsSync('build') || fs.mkdirSync('build');
        fs.mkdirSync('build/Release');
    }
    tres.pipe(require('fs').createWriteStream('build/Release/java.node'));
    tres.on('end', function () {
        console.log('binary installed');
    });
}).on('error', onerror).end();

function onerror(err) {
    console.error(err.message);
    console.error('binary installation failed, please rebuild it manually...');
}