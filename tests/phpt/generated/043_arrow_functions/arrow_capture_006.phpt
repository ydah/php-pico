--TEST--
arrow function implicit capture vector 006
--FILE--
<?php
$base = 25;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(3), ':', $add(9);
--EXPECT--
28:34
