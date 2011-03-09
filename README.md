php-fastcov 0.9.0
=================

This extension provides precise code coverage on PHP applications with as little overhead as possible ( 5% to 10% ).

### Current status: beta

Configuration
-------------

Possible .ini values are:
##### fastcov.auto_start On|Off [default: Off]
Automatically starts the coverage, you may provide either an output dir in `fastcov.output_dir` or use `fastcov_stop()` yourself
##### fastcov.auto_output On|Off [default: Off]
Automatically output the coverage data in a file when stopping code coverage or when the script is finished, `On` is implied if `fastcov.auto_start` is enabled
##### fastcov.output_dir "/path" [default: "/tmp"]
Output directory for coverage files, php must have write permission in this directory

Methods
-------

#### fastcov_start()
Starts the code coverage
#### fastcov_running()
Whether code coverage is running or not
#### fastcov_stop([bool force_output = false [ , bool no_return = false])
Stops the code coverage
If `force_output` is set to true, it will write the coverage encoded in json in a file whose format
is `fastcov-<random_sum>` where random_sum is a random md5sum; stored in the directory specified by the `fastcov.output_dir` ini directive ( by the "/tmp" folder ).
If `no_return` is set, the function return null ( allows to save memory if force_output is already set to true ).

Usage
-----

Basic code coverage can be done using the following lines:

    01. <?php
    02. 
    03. fastcov_start();
    04. 
    05. /* this is a sample file that show how fastcov
    06.  * does it's code coverage */
    07. 
    08. function addtwo($arg1) {
    09.     if (is_numeric($arg1)) {
    10.         return $arg1 + 2;
    11.     }
    12.     return false;
    13.     return 'dead code';
    14. }
    15. 
    16. /* setup some variables */
    17. $var2 = 'test';
    18. 
    19. // then call our functions
    20. var_dump(addtwo(7));
    21. var_dump(
    22.     addtwo($var2));
    23. 
    24. if (false) {
    25.     var_dump(addtwo(17));
    26. }
    27. 
    28. var_dump(fastcov_stop());

Based on this sample, `$coverage` will contains the following array,
it only sets to "1" lines that are executed at least once during the script execution:

    array(1) {
      ["/var/www/fastcov/coverage.php"]=>
      array(10) {
        [3]=>
        int(1)
        [9]=>
        int(1)
        [10]=>
        int(1)
        [12]=>
        int(1)
        [14]=>
        int(1)
        [17]=>
        int(1)
        [20]=>
        int(1)
        [22]=>
        int(1)
        [24]=>
        int(1)
        [28]=>
        int(1)
      }
    }
