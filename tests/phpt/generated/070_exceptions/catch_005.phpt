--TEST--
exception catch message and code vector 005
--FILE--
<?php
try { throw new Exception('caught-05', 38); }
catch (Exception $error) {
    echo $error->getMessage(), ':', $error->getCode();
}
--EXPECT--
caught-05:38
