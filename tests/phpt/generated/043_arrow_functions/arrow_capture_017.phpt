--TEST--
arrow function implicit capture vector 017
--FILE--
<?php
$base = 69;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(4), ':', $add(6);
--EXPECT--
73:75
