--TEST--
exception catch message and code vector 000
--FILE--
<?php
try { throw new Exception('caught-00', 3); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-00:3
