--TEST--
arrow function implicit capture vector 013
--FILE--
<?php
$base = 53;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(5), ':', $add(9);
--EXPECT--
58:62
