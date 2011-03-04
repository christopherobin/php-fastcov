php-fastcov 0.5
===============

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
    05. function do_something() {
    06.    echo "foo";
    07. }
    08. 
    09. do_something();
    10. do_something();
    11. 
    12. $coverage = fastcov_stop();

Based on this sample, `$coverage` will contains the following array:

    array(
        "/path/to/myfile.php" => array(
            03 => 1
            06 => 2,
            07 => 1,
            09 => 1,
            10 => 1
        )
    );
