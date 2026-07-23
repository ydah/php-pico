--TEST--
nested array copy-on-write vector 032
--FILE--
<?php
$tree = ['node' => ['value' => 129, 'items' => [130]]];
$copy = $tree;
$copy['node']['value'] += 6;
$copy['node']['items'][] = 137;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
129:135:130:130,137
