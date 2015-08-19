var uname = process.platform + '-' + process.arch + '-' + process.versions.modules + '.gz';

var pkg = require('./package.json');

require('http').get('http://' + pkg.author + '.github.io/' + pkg.name + '/dists/' + uname, function(tres) {
	console.log(tres.statusCode);
}).on('error', onerror);

function onerror(err) {
	console.error(err.message);
	console.error('binary installation failed, trying rebuild...');
}