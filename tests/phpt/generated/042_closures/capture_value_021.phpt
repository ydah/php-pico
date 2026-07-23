--TEST--
closure capture-by-value vector 021
--FILE--
<?php
$captured = 150;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(47);
--EXPECT--
197
