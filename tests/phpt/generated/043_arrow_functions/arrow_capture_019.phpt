--TEST--
arrow function implicit capture vector 019
--FILE--
<?php
$base = 77;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(6), ':', $add(8);
--EXPECT--
83:85
