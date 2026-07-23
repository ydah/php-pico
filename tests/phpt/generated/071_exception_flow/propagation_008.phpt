--TEST--
exception propagation through a function vector 008
--FILE--
<?php
function generated_throw_008($value) {
    if ($value > 0) { throw new RuntimeException('value=' . $value); }
    return 0;
}
try { generated_throw_008(76); }
catch (RuntimeException $error) { echo 'runtime:', $error->getMessage(); }
--EXPECT--
runtime:value=76
