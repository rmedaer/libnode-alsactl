
'use strict';
var assert = require('assert');
var binding = require('./build/Release/alsactl');
console.log('binding.hello() =', binding.list_controls(process.argv[2]));
