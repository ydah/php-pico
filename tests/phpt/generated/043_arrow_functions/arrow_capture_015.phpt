--TEST--
arrow function implicit capture vector 015
--FILE--
<?php
$base = 61;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(2), ':', $add(4);
--EXPECT--
63:65
