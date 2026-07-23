--TEST--
closure capture-by-value vector 022
--FILE--
<?php
$captured = 157;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(49);
--EXPECT--
206
