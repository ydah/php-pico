--TEST--
exception propagation through a function vector 000
--FILE--
<?php
function generated_throw_000($value) {
    if ($value > 0) { throw new RuntimeException('value=' . $value); }
    return 0;
}
try { generated_throw_000(4); }
catch (RuntimeException $error) { echo 'runtime:', $error->getMessage(); }
--EXPECT--
runtime:value=4
