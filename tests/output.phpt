--TEST--
Fastcov: Coverage Run with file output
Author: Christophe Robin
--FILE--
<?php

$odir = dirname(__FILE__);
ini_set('fastcov.output_dir', $odir);
fastcov_start(); // coverage is started from the following instruction

function do_something() {
    echo "foo\n";
}

do_something();
do_something();

// output to file and do not return anything
fastcov_stop(true, false);

// find file
$files = array();
foreach (scandir($odir) as $file) {
    $fullfile = $odir . DIRECTORY_SEPARATOR . $file;
    if (is_dir($fullfile) || (substr($file, 0, 8) != 'fastcov-')) continue;
    $files[$fullfile] = filemtime($fullfile);
}
arsort($files);
$files = array_keys($files);
$file = array_shift($files);

$coverage = file_get_contents($file);
unlink($file);
$odir = addcslashes($odir . DIRECTORY_SEPARATOR, '"/\\');
var_dump(str_replace($odir, '', $coverage));
--EXPECT--
foo
foo
string(48) "{"output.php":{"5":1,"8":2,"9":1,"11":1,"12":1}}"
