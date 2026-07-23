--TEST--
nested array copy-on-write vector 021
--FILE--
<?php
$tree = ['node' => ['value' => 85, 'items' => [86]]];
$copy = $tree;
$copy['node']['value'] += 2;
$copy['node']['items'][] = 89;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
85:87:86:86,89
