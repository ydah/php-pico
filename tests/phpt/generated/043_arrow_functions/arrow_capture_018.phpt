--TEST--
arrow function implicit capture vector 018
--FILE--
<?php
$base = 73;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(5), ':', $add(7);
--EXPECT--
78:80
