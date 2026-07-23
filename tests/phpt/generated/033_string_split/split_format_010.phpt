--TEST--
string split transform and padding vector 010
--FILE--
<?php
$parts = explode(',', 'v10,v11,v12');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX10'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v10|v11|v12:3:V11:mix10:v10----
