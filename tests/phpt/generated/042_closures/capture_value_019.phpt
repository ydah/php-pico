--TEST--
closure capture-by-value vector 019
--FILE--
<?php
$captured = 136;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(43);
--EXPECT--
179
