--TEST--
exception catch message and code vector 006
--FILE--
<?php
try { throw new Exception('caught-06', 45); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-06:45
