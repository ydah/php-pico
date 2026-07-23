--TEST--
JSON encode and decode vector 017
--FILE--
<?php
$value = ['name' => 'item17', 'number' => 222, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item17","number":222,"enabled":false}:item17:222:0
