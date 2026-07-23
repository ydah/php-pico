--TEST--
associative key order vector 025
--FILE--
<?php
$items = ['left' => 35, 4 => 95, 'right' => 155];
unset($items[4]);
$items['tail'] = 156;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:35,155,156:1
