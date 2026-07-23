--TEST--
exception catch message and code vector 001
--FILE--
<?php
try { throw new Exception('caught-01', 10); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-01:10
