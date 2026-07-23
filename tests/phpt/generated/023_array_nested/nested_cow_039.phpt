--TEST--
nested array copy-on-write vector 039
--FILE--
<?php
$tree = ['node' => ['value' => 157, 'items' => [158]]];
$copy = $tree;
$copy['node']['value'] += 6;
$copy['node']['items'][] = 165;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
157:163:158:158,165
