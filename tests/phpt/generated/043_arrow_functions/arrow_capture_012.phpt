--TEST--
arrow function implicit capture vector 012
--FILE--
<?php
$base = 49;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(4), ':', $add(8);
--EXPECT--
53:57
