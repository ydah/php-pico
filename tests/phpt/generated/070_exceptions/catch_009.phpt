--TEST--
exception catch message and code vector 009
--FILE--
<?php
try { throw new Exception('caught-09', 66); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-09:66
