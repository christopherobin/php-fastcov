--TEST--
Fastcov: Basic Coverage Run
Author: Christophe Robin
--FILE--
<?php

function foo($var) {
    $var = $var + 1;
}

$start = microtime(true);
$i = 100000;
while ($i--) {
    foo($i);
}
var_dump(microtime(true) - $start);

$start = microtime(true);
fastcov_start();
$i = 100000;
while ($i--) {
    foo($i);
}
var_dump(microtime(true) - $start);
var_dump(fastcov_stop());


--EXPECT--
foobar
array(1) {
  ["/home/crobin/Workspace/fastcov/tests/test.php"]=>
  array(3) {
    [4]=>
    NULL
    [5]=>
    NULL
    [6]=>
    NULL
  }
}
