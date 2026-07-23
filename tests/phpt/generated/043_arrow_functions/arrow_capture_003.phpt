--TEST--
arrow function implicit capture vector 003
--FILE--
<?php
$base = 13;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(5), ':', $add(6);
--EXPECT--
18:19
