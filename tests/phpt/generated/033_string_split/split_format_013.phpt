--TEST--
string split transform and padding vector 013
--FILE--
<?php
$parts = explode(',', 'v13,v14,v15');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX13'), ':', str_pad($parts[0], 6, '-');
--EXPECT--
v13|v14|v15:3:V14:mix13:v13---
