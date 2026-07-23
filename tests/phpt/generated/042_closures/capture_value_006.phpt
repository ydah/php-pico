--TEST--
closure capture-by-value vector 006
--FILE--
<?php
$captured = 45;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(17);
--EXPECT--
62
