--TEST--
exception catch message and code vector 008
--FILE--
<?php
try { throw new Exception('caught-08', 59); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-08:59
