--TEST--
arrow function implicit capture vector 008
--FILE--
<?php
$base = 33;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(5), ':', $add(4);
--EXPECT--
38:37
