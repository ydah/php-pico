--TEST--
arrow function implicit capture vector 011
--FILE--
<?php
$base = 45;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(3), ':', $add(7);
--EXPECT--
48:52
