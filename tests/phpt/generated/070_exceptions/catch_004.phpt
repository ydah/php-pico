--TEST--
exception catch message and code vector 004
--FILE--
<?php
try { throw new Exception('caught-04', 31); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-04:31
