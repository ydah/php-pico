--TEST--
exception catch message and code vector 002
--FILE--
<?php
try { throw new Exception('caught-02', 17); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-02:17
