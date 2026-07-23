--TEST--
associative key order vector 020
--FILE--
<?php
$items = ['left' => 30, 4 => 80, 'right' => 130];
unset($items[4]);
$items['tail'] = 131;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:30,130,131:1
