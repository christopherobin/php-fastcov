php-fastcov
===========

This extension provides precise code coverage on PHP applications with as little overhead as possible ( 5% to 10% ).

### Current status: beta

Methods
-------

#### fastcov_start()
Starts the code coverage
#### fastcov_running()
Whether code coverage is running or not
#### fastcov_stop()
Stops the code coverage and return the result

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

Based on this samble, `$coverage` will contains the following array:

    array(
        "/path/to/myfile.php" => array(
            03 => 1
            06 => 2,
            07 => 1,
            09 => 1,
            10 => 1
        )
    );
