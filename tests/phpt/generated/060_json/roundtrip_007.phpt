--TEST--
JSON encode and decode vector 007
--FILE--
<?php
$value = ['name' => 'item07', 'number' => 92, 'enabled' => false];
$json = json_encode($value); $decoded = json_decode($json, true);
echo $json, ':', $decoded['name'], ':', $decoded['number'], ':', ($decoded['enabled'] ? 1 : 0);
--EXPECT--
{"name":"item07","number":92,"enabled":false}:item07:92:0
