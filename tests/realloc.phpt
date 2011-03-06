--TEST--
Fastcov: Basic Coverage Run
Author: Christophe Robin
--FILE--
<?php

function test($value) {	echo "bug!\n"; var_dump($value); }
function test2($coverage) {
	$fcoverage = array();
	foreach ($coverage as $file => $lines) {
		$fcoverage[basename($file)] = $lines;
	}
	var_dump($fcoverage);
}
test(fastcov_start());
test2(fastcov_stop());

--EXPECT--
bug!
bool(true)
array(1) {
  ["realloc.php"]=>
  array(2) {
    [3]=>
    int(2)
    [11]=>
    int(1)
  }
}
