--TEST--
exception propagation through a function vector 003
--FILE--
<?php
function generated_throw_003($value) {
    if ($value > 0) { throw new RuntimeException('value=' . $value); }
    return 0;
}
try { generated_throw_003(31); }
catch (RuntimeException $error) { echo 'runtime:', $error->getMessage(); }
--EXPECT--
runtime:value=31
