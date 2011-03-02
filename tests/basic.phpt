--TEST--
Fastcov: Basic Coverage Run
Author: Christophe Robin
--FILE--
<?php

fastcov_start(); // coverage is started from the following instruction

function do_something() {
    echo "foo\n";
}

do_something();
do_something();

$coverage = fastcov_stop();

// rebuild file path to make it relative
$fcoverage = array();
foreach ($coverage as $file => $lines) {
    $fcoverage[basename($file)] = $lines;
}
var_dump($fcoverage);
--EXPECT--
foo
foo
array(1) {
  ["basic.php"]=>
  array(5) {
    [3]=>
    int(1)
    [6]=>
    int(2)
    [7]=>
    int(1)
    [9]=>
    int(1)
    [10]=>
    int(1)
  }
}
