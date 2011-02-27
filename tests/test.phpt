--TEST--
Fastcov: Basic Coverage Run
Author: Christophe Robin
--FILE--
<?php

// 1: Sanity test a simple profile run
fastcov_start();
echo "foo";
echo "bar\n";
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
