--TEST--
closure capture-by-value vector 023
--FILE--
<?php
$captured = 164;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(51);
--EXPECT--
215
