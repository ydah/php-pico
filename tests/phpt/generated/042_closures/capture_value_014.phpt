--TEST--
closure capture-by-value vector 014
--FILE--
<?php
$captured = 101;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(33);
--EXPECT--
134
