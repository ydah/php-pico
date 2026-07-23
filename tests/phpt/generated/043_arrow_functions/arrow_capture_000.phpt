--TEST--
arrow function implicit capture vector 000
--FILE--
<?php
$base = 1;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(2), ':', $add(3);
--EXPECT--
3:4
