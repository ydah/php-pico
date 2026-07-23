--TEST--
exception catch message and code vector 003
--FILE--
<?php
try { throw new Exception('caught-03', 24); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-03:24
