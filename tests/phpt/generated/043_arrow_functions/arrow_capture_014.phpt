--TEST--
arrow function implicit capture vector 014
--FILE--
<?php
$base = 57;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(6), ':', $add(3);
--EXPECT--
63:60
