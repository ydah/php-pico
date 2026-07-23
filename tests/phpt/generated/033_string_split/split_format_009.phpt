--TEST--
string split transform and padding vector 009
--FILE--
<?php
$parts = explode(',', 'v9,v10,v11');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX9'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v9|v10|v11:3:V10:mix9:v9---
