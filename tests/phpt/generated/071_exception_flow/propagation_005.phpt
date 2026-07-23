--TEST--
exception propagation through a function vector 005
--FILE--
<?php
function generated_throw_005($value) {
    if ($value > 0) { throw new RuntimeException('value=' . $value); }
    return 0;
}
try { generated_throw_005(49); }
catch (RuntimeException $error) { echo 'runtime:', $error->getMessage(); }
--EXPECT--
runtime:value=49
