--TEST--
exception catch message and code vector 007
--FILE--
<?php
try { throw new Exception('caught-07', 52); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-07:52
