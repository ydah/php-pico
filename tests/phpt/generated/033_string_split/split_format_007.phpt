--TEST--
string split transform and padding vector 007
--FILE--
<?php
$parts = explode(',', 'v7,v8,v9');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX7'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v7|v8|v9:3:V8:mix7:v7-----
